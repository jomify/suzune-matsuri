"""Create the opaque, textured toon prototype for Suzune and Tamamo in UE editor Python."""
import unreal

ROOT = '/Game/Matsuri/Materials'
MEL = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

def node(mat, kind, x, y, **values):
    result = MEL.create_material_expression(mat, kind, x, y)
    if not result:
        raise RuntimeError('Unable to create material node ' + str(kind))
    for name, value in values.items():
        result.set_editor_property(name, value)
    return result

def wire(source, output, target, input_name):
    if not MEL.connect_material_expressions(source, output, target, input_name):
        raise RuntimeError('Unable to connect toon material')

path = ROOT + '/M_MatsuriToon'
if unreal.EditorAssetLibrary.does_asset_exist(path):
    raise RuntimeError('Toon prototype already exists; refusing to overwrite it')
mat = tools.create_asset('M_MatsuriToon', ROOT, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property('used_with_skeletal_mesh', True)
texture = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, 0,
               parameter_name='Albedo', texture=unreal.load_asset('/Engine/EngineResources/WhiteSquareTexture'))
normal = node(mat, unreal.MaterialExpressionPixelNormalWS, -900, 300)
light = node(mat, unreal.MaterialExpressionConstant3Vector, -900, 500,
             constant=unreal.LinearColor(0.35, -0.35, 0.87, 1))
dot = node(mat, unreal.MaterialExpressionDotProduct, -650, 300)
wire(normal, '', dot, 'A')
wire(light, '', dot, 'B')
sat = node(mat, unreal.MaterialExpressionSaturate, -450, 300)
wire(dot, '', sat, '')
scale = node(mat, unreal.MaterialExpressionMultiply, -250, 300, const_b=3.0)
wire(sat, '', scale, 'A')
floor = node(mat, unreal.MaterialExpressionFloor, -50, 300)
wire(scale, '', floor, '')
bands = node(mat, unreal.MaterialExpressionMultiply, 150, 300, const_b=0.20)
wire(floor, '', bands, 'A')
ambient = node(mat, unreal.MaterialExpressionAdd, 350, 300, const_b=0.90)
wire(bands, '', ambient, 'A')
color = node(mat, unreal.MaterialExpressionMultiply, 550, 0)
wire(texture, 'RGB', color, 'A')
wire(ambient, '', color, 'B')
if not MEL.connect_material_property(color, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
    raise RuntimeError('Unable to connect emissive')
MEL.recompile_material(mat)
if not unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False):
    raise RuntimeError('Unable to save toon material')

for name, texture_path in [
    ('MI_Toon_Tamamo', '/Game/Matsuri/Tripo/Parts/SK_Boss_Tamamo_Idle/SK_Boss_Tamamo_idle/Textures/Color_b43afe42-c0a2-4342-87ab-baee5d4c4bf8'),
    ('MI_Toon_Suzune', '/Game/Matsuri/Tripo/Parts/SK_Hero_Suzune_Idle/SK_Hero_Suzune_idle/Textures/Color_36f91a8d-30dd-4f2d-94ef-660daf1718cc'),
]:
    if unreal.EditorAssetLibrary.does_asset_exist(ROOT + '/' + name):
        raise RuntimeError('Refusing to overwrite ' + name)
    albedo = unreal.load_asset(texture_path)
    if not albedo:
        raise RuntimeError('Missing texture ' + texture_path)
    instance = tools.create_asset(name, ROOT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(instance, mat)
    MEL.set_material_instance_texture_parameter_value(instance, 'Albedo', albedo)
    if not unreal.EditorAssetLibrary.save_loaded_asset(instance, only_if_is_dirty=False):
        raise RuntimeError('Unable to save ' + name)
    unreal.log_warning('TOON_CREATED ' + name)
